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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define URL_SIZE 2048

#if defined(__EMSCRIPTEN__)

#include <emscripten/fetch.h>

typedef struct
{
    HttpGetCallback callback;
    void* calldata;
} FetchData;

struct Net
{
    emscripten_fetch_attr_t attr;
};

static void downloadSucceeded(emscripten_fetch_t *fetch) 
{
    FetchData* data = (FetchData*)fetch->userData;

    HttpGetData getData = 
    {
        .type = HttpGetDone,
        .done = 
        {
            .size = fetch->numBytes,
            .data = (u8*)fetch->data,
        },
        .calldata = data->calldata,
        .url = fetch->url,
    };

    data->callback(&getData);

    free(data);

    emscripten_fetch_close(fetch);
}

static void downloadFailed(emscripten_fetch_t *fetch) 
{
    FetchData* data = (FetchData*)fetch->userData;

    HttpGetData getData = 
    {
        .type = HttpGetError,
        .error = 
        {
            .code = fetch->status,
        },
        .calldata = data->calldata,
        .url = fetch->url,
    };

    data->callback(&getData);

    free(data);

    emscripten_fetch_close(fetch);
}

static void downloadProgress(emscripten_fetch_t *fetch) 
{
    FetchData* data = (FetchData*)fetch->userData;

    HttpGetData getData = 
    {
        .type = HttpGetProgress,
        .progress = 
        {
            .size = fetch->dataOffset + fetch->numBytes,
            .total = fetch->totalBytes,
        },
        .calldata = data->calldata,
        .url = fetch->url,
    };

    data->callback(&getData);
}

#else

#include <curl/curl.h>

typedef struct
{
    u8* buffer;
    s32 size;

    struct Curl_easy* async;
    HttpGetCallback callback;
    void* calldata;
    char url[URL_SIZE];
} CurlData;

struct Net
{
    const char* host;
    CURLM* multi;
    struct Curl_easy* sync;
};

static size_t writeCallbackSync(void *contents, size_t size, size_t nmemb, void *userp)
{
    CurlData* data = (CurlData*)userp;

    const size_t total = size * nmemb;
    u8* newBuffer = realloc(data->buffer, data->size + total);
    if (newBuffer == NULL)
    {
        free(data->buffer);
        return 0;
    }
    data->buffer = newBuffer;
    memcpy(data->buffer + data->size, contents, total);
    data->size += (s32)total;

    return total;
}

static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    CurlData* data = (CurlData*)userdata;

    const size_t total = size * nmemb;
    u8* newBuffer = realloc(data->buffer, data->size + total);
    if (newBuffer == NULL)
    {
        free(data->buffer);
        return 0;
    }
    data->buffer = newBuffer;
    memcpy(data->buffer + data->size, ptr, total);
    data->size += (s32)total;

    double cl;
    curl_easy_getinfo(data->async, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);

    if(cl > 0.0)
    {
        HttpGetData getData = 
        {
            .type = HttpGetProgress,
            .progress = 
            {
                .size = data->size,
                .total = (s32)cl,
            },
            .calldata = data->calldata,
            .url = data->url,
        };

        data->callback(&getData);
    }

    return total;
}

#endif

void netGet(Net* net, const char* path, HttpGetCallback callback, void* calldata)
{
#if defined(__EMSCRIPTEN__)

    FetchData* data = calloc(1, sizeof(FetchData));
    *data = (FetchData)
    {
        .callback = callback,
        .calldata = calldata,
    };

    net->attr.userData = data;
    emscripten_fetch(&net->attr, path);

    #else

    struct Curl_easy* curl = curl_easy_init();

    CurlData* data = calloc(1, sizeof(CurlData));
    *data = (CurlData)
    {
        .async = curl,
        .callback = callback,
        .calldata = calldata,
    };

    strcpy(data->url, path);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    {
        char url[URL_SIZE];
        strcpy(url, net->host);
        strcat(url, path);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        curl_easy_setopt(curl, CURLOPT_PRIVATE, data);

        curl_multi_add_handle(net->multi, curl);
    }

#endif
}

void* netGetSync(Net* net, const char* path, s32* size)
{
#ifdef DISABLE_NETWORKING

    return NULL;

#else
    #if defined(__EMSCRIPTEN__)

    return NULL;

    #else

    CurlData data = {NULL, 0};

    if(net->sync)
    {
        char url[URL_SIZE];
        strcpy(url, net->host);
        strcat(url, path);

        curl_easy_setopt(net->sync, CURLOPT_URL, url);
        curl_easy_setopt(net->sync, CURLOPT_WRITEDATA, &data);

        if(curl_easy_perform(net->sync) == CURLE_OK)
        {
            long httpCode = 0;
            curl_easy_getinfo(net->sync, CURLINFO_RESPONSE_CODE, &httpCode);
            if(httpCode != 200) return NULL;
        }
        else return NULL;
    }

    *size = data.size;

    return data.buffer;

    #endif
#endif
}

void netTick(Net *net)
{
#if !defined(__EMSCRIPTEN__)

    {
        s32 running = 0;
        curl_multi_perform(net->multi, &running);
    }

    s32 pending = 0;
    CURLMsg* msg = NULL;

    while((msg = curl_multi_info_read(net->multi, &pending)))
    {
        if(msg->msg == CURLMSG_DONE)
        {
            CurlData* data = NULL;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, (char**)&data);

            long httpCode = 0;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &httpCode);

            if(httpCode == 200)
            {
                HttpGetData getData = 
                {
                    .type = HttpGetDone,
                    .done = 
                    {
                        .size = data->size,
                        .data = data->buffer,
                    },
                    .calldata = data->calldata,
                    .url = data->url,
                };

                data->callback(&getData);

                free(data->buffer);
            }
            else
            {
                HttpGetData getData = 
                {
                    .type = HttpGetError,
                    .error = 
                    {
                        .code = httpCode,
                    },
                    .calldata = data->calldata,
                    .url = data->url,
                };

                data->callback(&getData);
            }

            free(data);
            
            curl_multi_remove_handle(net->multi, msg->easy_handle);
            curl_easy_cleanup(msg->easy_handle);
        }
    }
#endif
}

Net* createNet(const char* host)
{
    Net* net = (Net*)malloc(sizeof(Net));

    #if defined(__EMSCRIPTEN__)

    emscripten_fetch_attr_init(&net->attr);
    strcpy(net->attr.requestMethod, "GET");
    net->attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    net->attr.onsuccess = downloadSucceeded;
    net->attr.onerror = downloadFailed;
    net->attr.onprogress = downloadProgress;

    #else

    if (net != NULL)
    {
        *net = (Net)
        {
            .sync = curl_easy_init(),
            .multi = curl_multi_init(),
            .host = host,
        };

        curl_easy_setopt(net->sync, CURLOPT_WRITEFUNCTION, writeCallbackSync);
    }

    #endif

    return net;
}

void closeNet(Net* net)
{
    #if !defined(__EMSCRIPTEN__)
    if(net->sync)
        curl_easy_cleanup(net->sync);

    if(net->multi)
        curl_multi_cleanup(net->multi);

    #endif

    free(net);
}
