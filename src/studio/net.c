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

#if defined(__ANDROID__)
#include "system.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define URL_SIZE 2048

#if defined(NET_TEST)

// The transport contract's test provides tic_net_request itself (tests/net.c);
// this file supplies the get bridge that is exercised through it.

#elif defined(__3DS__) || defined(__SWITCH__)

// See net.c in src/system

#elif defined(__EMSCRIPTEN__)

#include <emscripten/fetch.h>

// A request the server never answers must fail on its own call: with no
// timeout a stalled link holds a browser connection slot for minutes, and
// every other request of the studio queues behind it.
#define FETCH_TIMEOUT_MSEC 15000

// Only the small requests of the studio — the listings, the covers and the
// pairing — carry that deadline. A cartridge is what the user waits for and
// can be megabytes, and the deadline is a total one, so a slow download and
// an upload must be left alone.
static bool isShortRequest(net_method method, const char* path)
{
    if(strncmp(path, "/json", 5) == 0 || strstr(path, "cover.gif") != NULL)
        return true;

    static const char ShelfApi[] = "/api/v1/client/shelf";
    static const char LikesApi[] = "/api/v1/client/likes";
    static const char DeviceApi[] = "/api/v1/client/device";
    const s32 shelfLen = (s32)sizeof ShelfApi - 1;
    const s32 likesLen = (s32)sizeof LikesApi - 1;
    const s32 deviceLen = (s32)sizeof DeviceApi - 1;

    // The likes listing is small like the shelf's.
    if(method == net_method_get && strncmp(path, LikesApi, likesLen) == 0 && path[likesLen] == '\0')
        return true;

    // The listing and the delete; a shelf cart's GET and PUT are the large ones.
    if(method == net_method_get && strncmp(path, ShelfApi, shelfLen) == 0)
        return path[shelfLen] == '\0';

    if(method == net_method_delete && strncmp(path, ShelfApi, shelfLen) == 0 && path[shelfLen] == '/')
        return true;

    // The pairing start and its poll.
    if(method == net_method_post && strncmp(path, DeviceApi, deviceLen) == 0 && path[deviceLen] == '\0')
        return true;

    if(method == net_method_get && strncmp(path, DeviceApi, deviceLen) == 0 && path[deviceLen] == '/')
        return true;

    return false;
}

typedef struct
{
    net_request_callback callback;
    void* calldata;
    const char** headers; // the fetch's key/value pairs, live until the answer
} FetchData;

struct tic_net
{
    emscripten_fetch_attr_t attr;
};

static void freeFetchData(FetchData* data)
{
    free((void*)data->headers);
    free(data);
}

static void downloadSucceeded(emscripten_fetch_t* fetch)
{
    FetchData* data = (FetchData*)fetch->userData;

    net_request_data requestData =
        {
            .type = net_request_done,
            .done =
                {
                    .status = fetch->status,
                    .size = fetch->numBytes,
                    .data = (u8*)fetch->data,
                },
            .calldata = data->calldata,
            .url = fetch->url,
        };

    data->callback(&requestData);

    // The body lives with the fetch and emscripten_fetch_close() frees it:
    // freeing it here as well was a double free on every answered request,
    // which is what a release dlmalloc eats in silence.
    freeFetchData(data);

    emscripten_fetch_close(fetch);
}

static void downloadFailed(emscripten_fetch_t* fetch)
{
    FetchData* data = (FetchData*)fetch->userData;

    // An HTTP answer is completed whether it says 200 or 500: only a fetch
    // without a status is a transport failure.
    net_request_data requestData =
        {
            .calldata = data->calldata,
            .url = fetch->url,
        };

    if(fetch->status > 0)
    {
        requestData.type = net_request_done;
        requestData.done.status = fetch->status;
        requestData.done.size = fetch->numBytes;
        requestData.done.data = (u8*)fetch->data;
    }
    else
    {
        requestData.type = net_request_error;
        requestData.error.code = fetch->status;
    }

    data->callback(&requestData);

    freeFetchData(data);

    emscripten_fetch_close(fetch);
}

static void downloadProgress(emscripten_fetch_t* fetch)
{
    FetchData* data = (FetchData*)fetch->userData;

    net_request_data requestData =
        {
            .type = net_request_progress,
            .progress =
                {
                    .size = fetch->dataOffset + fetch->numBytes,
                    .total = fetch->totalBytes,
                },
            .calldata = data->calldata,
            .url = fetch->url,
        };

    data->callback(&requestData);
}

static const char* methodName(net_method method)
{
    switch(method)
    {
        case net_method_post:
            return "POST";
        case net_method_put:
            return "PUT";
        case net_method_delete:
            return "DELETE";
        default:
            return "GET";
    }
}

// The primitive's method, header and body half over the browser: the cookie
// session of a same-origin site rides along with withCredentials, the header
// array is key/value pairs as emscripten_fetch wants them, and the body is the
// caller's bytes, which it keeps until the answer.
void tic_net_request(tic_net* net, net_method method, const char* path, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    FetchData* data = calloc(1, sizeof(FetchData));

    if(!data)
    {
        net_request_data requestData =
            {
                .type = net_request_error,
                .error = {.code = -1},
                .calldata = calldata,
                .url = path,
            };

        callback(&requestData);
        return;
    }

    data->callback = callback;
    data->calldata = calldata;

    if(headersCount > 0)
    {
        data->headers = calloc((size_t)headersCount * 2 + 1, sizeof(char*));

        if(!data->headers)
        {
            net_request_data requestData =
                {
                    .type = net_request_error,
                    .error = {.code = -1},
                    .calldata = calldata,
                    .url = path,
                };

            callback(&requestData);
            freeFetchData(data);
            return;
        }

        for(s32 i = 0; i < headersCount; i++)
        {
            data->headers[i * 2] = headers[i].name;
            data->headers[i * 2 + 1] = headers[i].value;
        }
    }

    // The fetch copies the attr when it starts, so the shared one is safe:
    // concurrent requests keep their own method, headers, body and userData.
    emscripten_fetch_attr_t* attr = &net->attr;
    emscripten_fetch_attr_init(attr);
    strcpy(attr->requestMethod, methodName(method));
    attr->attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr->onsuccess = downloadSucceeded;
    attr->onerror = downloadFailed;
    attr->onprogress = downloadProgress;
    attr->withCredentials = 1;
    attr->userData = data;
    attr->requestHeaders = data->headers;
    attr->requestData = body;
    attr->requestDataSize = bodySize;
    attr->timeoutMSecs = isShortRequest(method, path) ? FETCH_TIMEOUT_MSEC : 0;

    // A fetch that could not even start answers like a failed one, so no
    // caller is left waiting on a callback that will never come.
    if(!emscripten_fetch(attr, path))
    {
        net_request_data requestData =
            {
                .type = net_request_error,
                .error = {.code = -1},
                .calldata = calldata,
                .url = path,
            };

        callback(&requestData);
        freeFetchData(data);
    }
}

void tic_net_start(tic_net* net)
{
    (void)net;
}
void tic_net_end(tic_net* net)
{
    (void)net;
}

tic_net* tic_net_create(const char* host)
{
    (void)host;
    tic_net* net = (tic_net*)calloc(1, sizeof(tic_net));

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

    net_request_callback callback;
    void* calldata;
} HttpRequest;

struct tic_net
{
    char host[URL_SIZE];

    HttpRequest** requests;
    s32 count;
};

static const char* methodName(net_method method)
{
    switch(method)
    {
        case net_method_post:
            return "POST";
        case net_method_put:
            return "PUT";
        case net_method_delete:
            return "DELETE";
        default:
            return "GET";
    }
}

tic_net* tic_net_create(const char* host)
{
#if defined(__ANDROID__)
    naettInit(tic_sys_java_vm());
#else
    naettInit(NULL);
#endif

    tic_net* net = NEW(tic_net);
    memset(net, 0, sizeof(tic_net));

    strcpy(net->host, host);

    return net;
}

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    HttpRequest* request = NEW(HttpRequest);
    memset(request, 0, sizeof *request);

    snprintf(request->url, sizeof request->url, "%s%s", net->host, url);

    // The options are consumed (and freed) by naettRequestWithOptions.
    s32 optionsCount = 2 + headersCount + (body ? 1 : 0);
    const naettOption** options = calloc(optionsCount, sizeof *options);
    options[0] = naettMethod(methodName(method));
    options[1] = naettHeader("accept", "*/*");
    for(s32 i = 0; i < headersCount; i++)
    {
        options[2 + i] = naettHeader(headers[i].name, headers[i].value);
    }
    if(body)
    {
        options[2 + headersCount] = naettBody(body, bodySize);
    }

    request->req = naettRequestWithOptions(request->url, optionsCount, options);
    free(options);

    if(request->req == NULL)
    {
        net_request_data requestData =
            {
                .type = net_request_error,
                .error = {.code = -1},
                .calldata = calldata,
                .url = request->url,
            };

        callback(&requestData);
        free(request);
        return;
    }

    request->res = naettMake(request->req);
    request->callback = callback;
    request->calldata = calldata;

    net->requests = realloc(net->requests, sizeof *net->requests * ++net->count);
    net->requests[net->count - 1] = request;
}

void tic_net_close(tic_net* net)
{
    for(s32 i = 0; i < net->count; i++)
    {
        HttpRequest* it = net->requests[i];

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

void tic_net_start(tic_net* net)
{
    (void)net;
}

void tic_net_end(tic_net* net)
{
    if(!net->requests)
        return;

    for(s32 i = 0; i < net->count; i++)
    {
        const HttpRequest* it = net->requests[i];

        if(it && naettComplete(it->res))
        {
            s32 status = naettGetStatus(it->res);

            net_request_data requestData =
                {
                    .calldata = it->calldata,
                    .url = it->url,
                };

            if(status > 0)
            {
                requestData.type = net_request_done;
                requestData.done.status = status;
                requestData.done.data = (u8*)naettGetBody(it->res, &requestData.done.size);
            }
            else
            {
                requestData.type = net_request_error;
                requestData.error.code = status;
            }

            it->callback(&requestData);

            naettClose(it->res);
            naettFree(it->req);

            free(net->requests[i]);
            net->requests[i] = NULL;
        }
    }
}

#elif defined(__SWITCH__) || defined(__TANMATSU__)

// See net.c in src/system

#else

tic_net* tic_net_create(const char* host)
{
    (void)host;
    return NULL;
}
void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    (void)method;
    (void)headers;
    (void)headersCount;
    (void)body;
    (void)bodySize;

    // close all requests with error to keep frontend responsive
    net_request_data requestData =
        {
            .type = net_request_error,
            .calldata = calldata,
            .url = url,
            .error.code = -1, // abitrary
        };
    callback(&requestData);
}
void tic_net_close(tic_net* net)
{
    (void)net;
}
void tic_net_start(tic_net* net)
{
    (void)net;
}
void tic_net_end(tic_net* net)
{
    (void)net;
}

#endif

// tic_net_get is the site mount's old shape kept on top of the primitive: a
// 200 is done, any other status is its error code, and an error's body is
// dropped. The bridge lives with the call so the two cannot disagree about the
// translation, and every backend gets it from one place.
typedef struct
{
    net_get_callback callback;
    void* calldata;
} GetBridge;

static void bridgeRequest(const net_request_data* requestData)
{
    GetBridge* bridge = requestData->calldata;

    net_get_data getData = {.calldata = bridge->calldata, .url = requestData->url};

    switch(requestData->type)
    {
        case net_request_progress:
            getData.type = net_get_progress;
            getData.progress.size = requestData->progress.size;
            getData.progress.total = requestData->progress.total;
            bridge->callback(&getData);
            break;
        case net_request_done:
            if(requestData->done.status == 200)
            {
                getData.type = net_get_done;
                getData.done.size = requestData->done.size;
                getData.done.data = requestData->done.data;
            }
            else
            {
                getData.type = net_get_error;
                getData.error.code = requestData->done.status;
            }
            bridge->callback(&getData);
            free(bridge);
            break;
        case net_request_error:
            getData.type = net_get_error;
            getData.error.code = requestData->error.code;
            bridge->callback(&getData);
            free(bridge);
            break;
    }
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    GetBridge* bridge = calloc(1, sizeof(GetBridge));
    bridge->callback = callback;
    bridge->calldata = calldata;

    tic_net_request(net, net_method_get, url, NULL, 0, NULL, 0, bridgeRequest, bridge);
}
