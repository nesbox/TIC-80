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
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define URL_SIZE 2048

#if defined(__3DS__) || defined(__SWITCH__)

// See net.c in src/system

#elif defined(__EMSCRIPTEN__)

#include <emscripten/fetch.h>

typedef struct
{
    net_get_callback callback;
    void* calldata;
} FetchData;

struct tic_net
{
    emscripten_fetch_attr_t attr;
};

static void downloadSucceeded(emscripten_fetch_t *fetch)
{
    FetchData* data = (FetchData*)fetch->userData;

    net_get_data getData =
    {
        .type = net_get_done,
        .done =
        {
            .size = fetch->numBytes,
            .data = (u8*)fetch->data,
        },
        .calldata = data->calldata,
        .url = fetch->url,
    };

    data->callback(&getData);

    free((void*)fetch->data);
    free(data);

    emscripten_fetch_close(fetch);
}

static void downloadFailed(emscripten_fetch_t *fetch)
{
    FetchData* data = (FetchData*)fetch->userData;

    net_get_data getData =
    {
        .type = net_get_error,
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

    net_get_data getData =
    {
        .type = net_get_progress,
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

void tic_net_get(tic_net* net, const char* path, net_get_callback callback, void* calldata)
{
    FetchData* data = calloc(1, sizeof(FetchData));
    *data = (FetchData)
    {
        .callback = callback,
        .calldata = calldata,
    };

    net->attr.userData = data;
    emscripten_fetch(&net->attr, path);
}

void tic_net_start(tic_net *net) {}
void tic_net_end(tic_net *net) {}

tic_net* tic_net_create(const char* host)
{
    tic_net* net = (tic_net*)malloc(sizeof(tic_net));

    emscripten_fetch_attr_init(&net->attr);
    strcpy(net->attr.requestMethod, "GET");
    net->attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    net->attr.onsuccess = downloadSucceeded;
    net->attr.onerror = downloadFailed;
    net->attr.onprogress = downloadProgress;

    return net;
}

void tic_net_close(tic_net* net)
{
    free(net);
}

#elif defined(USE_NAETT)

#include <naett.h>

typedef struct
{
    naettReq* req;
    naettRes* res;
    char url[URL_SIZE];

    net_get_callback callback;
    void* calldata;
} HttpGet;

struct tic_net
{
    char host[URL_SIZE];

    HttpGet** requests;
    s32 count;
};

#if defined(__ANDROID__)
#include <jni.h>
JNIEnv *Android_JNI_GetEnv();
#endif

tic_net* tic_net_create(const char* host)
{
#if defined(__ANDROID__)
    JNIEnv *env = Android_JNI_GetEnv();
    JavaVM *vm = NULL;
    (*env)->GetJavaVM(env, &vm);

    naettInit(vm);
#else
    naettInit(NULL);
#endif

    tic_net* net = NEW(tic_net);
    memset(net, 0, sizeof(tic_net));

    strcpy(net->host, host);

    return net;
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    HttpGet* get = NEW(HttpGet);
    memset(get, 0, sizeof *get);

    sprintf(get->url, "%s%s", net->host, url);

    get->req = naettRequest(get->url, naettMethod("GET"), naettHeader("accept", "*/*"));
    get->res = naettMake(get->req);
    get->callback = callback;
    get->calldata = calldata;

    net->requests = realloc(net->requests, sizeof *net->requests * ++net->count);
    net->requests[net->count - 1] = get;
}

void tic_net_close(tic_net* net)
{
    for(s32 i = 0; i < net->count; i++)
    {
        HttpGet *it = net->requests[i];

        if(it)
        {
            naettClose(it->res);
            naettFree(it->req);
            free(it);
        }
    }

    if(net->requests)
        free(net->requests);
}

void tic_net_start(tic_net *net) {}

void tic_net_end(tic_net *net)
{
    if(!net->requests)
        return;

    for(s32 i = 0; i < net->count; i++)
    {
        const HttpGet *it = net->requests[i];

        if(it && naettComplete(it->res))
        {
            s32 status = naettGetStatus(it->res);

            net_get_data getData =
            {
                .calldata = it->calldata,
                .url = it->url,
            };

            if(status == 200)
            {
                getData.type = net_get_done;
                getData.done.data = (u8*)naettGetBody(it->res, &getData.done.size);
            }
            else
            {
                getData.type = net_get_error;
                getData.error.code = status;
            }

            it->callback(&getData);

            naettClose(it->res);
            naettFree(it->req);

            free(net->requests[i]);
            net->requests[i] = NULL;
        }
    }
}

#elif defined(__SWITCH__)

// See net.c in src/system

#else

tic_net* tic_net_create(const char* host) {return NULL;}
void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    // close all requests with error to keep frontend responsive
    net_get_data getData =
    {
        .type = net_get_error,
        .calldata = calldata,
        .url = url,
        .error.code = -1, // abitrary
    };
    callback(&getData);
}
void tic_net_close(tic_net* net) {}
void tic_net_start(tic_net *net) {}
void tic_net_end(tic_net *net) {}

#endif
