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

#if defined(__EMSCRIPTEN__)

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

#elif defined(_3DS)

#include <3ds.h>

struct tic_net
{
    LightLock tick_lock;
    const char* host;
};

typedef struct {
    char url[URL_SIZE];

    tic_net *net;
    httpcContext httpc;
    net_get_data data;
    net_get_callback callback;

    void *buffer;
    s32 size;
} net_ctx;

// #define DEBUG

static int n3ds_net_setup_context(httpcContext *httpc, const char *url) {
    if (httpcOpenContext(httpc, HTTPC_METHOD_GET, url, 1)) return -101;
    if (httpcSetSSLOpt(httpc, SSLCOPT_DisableVerify)) { httpcCloseContext(httpc); return -102; }
    if (httpcSetKeepAlive(httpc, HTTPC_KEEPALIVE_ENABLED)) { httpcCloseContext(httpc); return -103; }
    if (httpcAddRequestHeaderField(httpc, "User-Agent", "tic80-n3ds/1.0.0 (httpc)")) { httpcCloseContext(httpc); return -104; }
    if (httpcAddRequestHeaderField(httpc, "Connection", "Keep-Alive")) { httpcCloseContext(httpc); return -105; }
    return 0;
}

#define NET_PAGE_SIZE 4096

static inline bool ctx_resize_buf(net_ctx *ctx, s32 new_size) {
    if (ctx->buffer == NULL) {
        ctx->buffer = malloc(new_size);
        if (ctx->buffer == NULL) {
            return false;
        }
    } else if (ctx->size != new_size) {
        void *old_buf = ctx->buffer;
        ctx->buffer = realloc(ctx->buffer, new_size);
        if (ctx->buffer == NULL) {
            free(old_buf);
            return false;
        }
    }

    ctx->size = new_size;
    return true;
}

#define NET_EXEC_ERROR_CHECK \
    if (status_code != 200) { \
        printf("net_httpc: error %d\n", status_code); \
        if (ctx->callback != NULL) { \
            ctx->data.type = net_get_error; \
            ctx->data.error.code = status_code; \
            if (!ignore_lock) LightLock_Lock(&ctx->net->tick_lock); \
            ctx->callback(&ctx->data); \
            if (!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock); \
        } \
        httpcCloseContext(&ctx->httpc); \
        if (ctx->buffer != NULL) { free(ctx->buffer); ctx->size = 0; } \
        return; \
    }

static void n3ds_net_execute(net_ctx *ctx, bool ignore_lock) {
    bool redirecting = true;
    s32 status_code = -1;

    ctx->data.url = ctx->url;
    while (redirecting) {
#ifdef DEBUG
        printf("url: %s\n", ctx->url);
#endif
        redirecting = false;

        status_code = n3ds_net_setup_context(&ctx->httpc, ctx->url);
        if (status_code < 0) {
            break;
        }

        if (httpcBeginRequest(&ctx->httpc)) {
            status_code = -2;
            break;
        }

        if (httpcGetResponseStatusCode(&ctx->httpc, &status_code)) {
            status_code = -3;
            break;
        }

        if ((status_code >= 301 && status_code <= 303) || (status_code >= 307 && status_code <= 308)) {
            if (httpcGetResponseHeader(&ctx->httpc, "Location", ctx->url, URL_SIZE - 1)) {
                status_code = -4;
                break;
            }

            redirecting = true;
            httpcCloseContext(&ctx->httpc);
        }
    }

    NET_EXEC_ERROR_CHECK;

    s32 state = HTTPC_RESULTCODE_DOWNLOADPENDING;
    s32 read_size;
    while (state == HTTPC_RESULTCODE_DOWNLOADPENDING) {
        s32 old_size = ctx->size;
        if (!ctx_resize_buf(ctx, ctx->size + NET_PAGE_SIZE)) {
            httpcCloseContext(&ctx->httpc);
            status_code = -5;
            break;
        }
        u8 *old_ptr = ((u8*) ctx->buffer) + old_size;
        state = httpcDownloadData(&ctx->httpc, old_ptr, NET_PAGE_SIZE, &read_size);
        if (state == HTTPC_RESULTCODE_DOWNLOADPENDING || state == 0) {
            ctx_resize_buf(ctx, old_size + read_size);
            if (ctx->callback != NULL) {
                if (ignore_lock || !LightLock_TryLock(&ctx->net->tick_lock)) {
                    ctx->data.type = net_get_progress;
                    if (!httpcGetDownloadSizeState(&ctx->httpc, &ctx->data.progress.size, &ctx->data.progress.total)) {
                        if (ctx->data.progress.total < ctx->data.progress.size) {
                            ctx->data.progress.total = ctx->data.progress.size;
                        }
                        ctx->callback(&ctx->data);
                    }
                    if (!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock);
                }
            }
        }
    }

#ifdef DEBUG
    printf("downloaded: %d bytes\n", ctx->size);
#endif

    if (status_code == 200 && state != 0) {
        status_code = -6;
    }
    NET_EXEC_ERROR_CHECK;

    if (ctx->callback != NULL) {
        ctx->data.type = net_get_done;
        ctx->data.done.data = ctx->buffer;
        ctx->data.done.size = ctx->size;
        if (!ignore_lock) LightLock_Lock(&ctx->net->tick_lock);
        ctx->callback(&ctx->data);
        if (!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock);
    }
    httpcCloseContext(&ctx->httpc);
}

static void n3ds_net_init(tic_net *net) {
    httpcInit(0);

    memset(net, 0, sizeof(tic_net));
    LightLock_Init(&net->tick_lock);
}

static void n3ds_net_free(tic_net *net) {
    httpcExit();
}

static void n3ds_net_get_thread(net_ctx *ctx) {
    n3ds_net_execute(ctx, false);

    if (ctx->buffer != NULL) {
        free(ctx->buffer);
    }
    free(ctx);
}

static void n3ds_net_apply_url(net_ctx *ctx, const char *url) 
{
    strncpy(ctx->url, "http://", URL_SIZE - 1);
    strncat(ctx->url, ctx->net->host, URL_SIZE - 1);
    strncat(ctx->url, url, URL_SIZE - 1);
}

tic_net* tic_net_create(const char* host)
{
    tic_net* net = (tic_net*)malloc(sizeof(tic_net));

    n3ds_net_init(net);
    net->host = host;

    static const char Http[] = "http://";
    if(strstr(host, Http) == host)
        net->host += STRLEN(Http);

    static const char Https[] = "https://";
    if(strstr(host, Https) == host)
        net->host += STRLEN(Https);

    return net;
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    net_ctx ctx = 
    {
        .net = net,
        .callback = callback,
        .data = {.calldata = calldata},
    };

    n3ds_net_apply_url(&ctx, url);

    s32 priority;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    threadCreate((ThreadFunc) n3ds_net_get_thread, MOVE(ctx), 16 * 1024, priority - 1, -1, true);
}

void tic_net_close(tic_net* net)
{
    n3ds_net_free(net);
    free(net);
}

void tic_net_start(tic_net *net)
{
    LightLock_Lock(&net->tick_lock);
}

void tic_net_end(tic_net *net)
{
    LightLock_Unlock(&net->tick_lock);
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

#else

tic_net* tic_net_create(const char* host) {return NULL;}
void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata) {}
void tic_net_close(tic_net* net) {}
void tic_net_start(tic_net *net) {}
void tic_net_end(tic_net *net) {}

#endif
