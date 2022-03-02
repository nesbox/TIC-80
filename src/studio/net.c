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

static void n3ds_net_apply_url(net_ctx *ctx, const char *url) {
    strncpy(ctx->url, ctx->net->host, URL_SIZE - 1);
    strncat(ctx->url, url, URL_SIZE - 1);
}

tic_net* tic_net_create(const char* host)
{
    tic_net* net = (tic_net*)malloc(sizeof(tic_net));

    n3ds_net_init(net);
    net->host = host;

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

#elif defined(BAREMETALPI)

tic_net* tic_net_create(const char* host) {return NULL;}
void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata) {}
void tic_net_close(tic_net* net) {}
void tic_net_start(tic_net *net) {}
void tic_net_end(tic_net *net) {}

#elif defined(USE_LIBUV)

#include "defines.h"

#include <uv.h>
#include <http_parser.h>

typedef struct
{
    const char* host;
    const char* path;

    net_get_callback callback;
    void* calldata;

    uv_tcp_t tcp;
    http_parser parser;

    struct
    {
        u8* data;
        s32 size;
        s32 total;
    } content;

} NetRequest;

struct tic_net
{
    const char* host;
};

static s32 onBody(http_parser* parser, const char *at, size_t length)
{
    NetRequest* req = parser->data;

    req->content.data = realloc(req->content.data, req->content.size + length);
    memcpy(req->content.data + req->content.size, at, length);

    req->content.size += length;

    req->callback(&(net_get_data) 
    {
        .calldata = req->calldata, 
        .type = net_get_progress, 
        .progress = {req->content.size, req->content.total}, 
        .url = req->path
    });

    return 0;
}

static void onClose(uv_handle_t* handle)
{
    NetRequest* req = handle->data;
    FREE(req->content.data);
    free((void*)req->path);
    free(req);
}

static void freeRequest(NetRequest* req)
{
    uv_close((uv_handle_t*)&req->tcp, onClose);
}

static s32 onMessageComplete(http_parser* parser)
{
    NetRequest* req = parser->data;

    if (parser->status_code == HTTP_STATUS_OK)
    {
        req->callback(&(net_get_data)
        {
            .calldata = req->calldata,
            .type = net_get_done,
            .done = { .data = req->content.data, .size = req->content.size },
            .url = req->path
        });
    }

    freeRequest(req);

    return 0;
}

static s32 onHeadersComplete(http_parser* parser)
{
    NetRequest* req = parser->data;

    bool hasBody = parser->flags & F_CHUNKED || (parser->content_length > 0 && parser->content_length != ULLONG_MAX);

    ZEROMEM(req->content);
    req->content.total = parser->content_length;

    // !TODO: handle HTTP_STATUS_MOVED_PERMANENTLY here
    if (!hasBody || parser->status_code != HTTP_STATUS_OK)
        return 1;

    return 0;
}

static s32 onStatus(http_parser* parser, const char* at, size_t length)
{
    return parser->status_code != HTTP_STATUS_OK;
}

static void onError(NetRequest* req, s32 code)
{
    req->callback(&(net_get_data)
    {
        .calldata = req->calldata,
        .type = net_get_error,
        .error = { .code = code }
    });

    freeRequest(req);
}

static void allocBuffer(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = malloc(size);
    buf->len = size;
}

static void onResponse(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) 
{
    NetRequest* req = stream->data;

    if(nread > 0)
    {
        static const http_parser_settings ParserSettings = 
        {
            .on_status = onStatus,
            .on_body = onBody,
            .on_message_complete = onMessageComplete,
            .on_headers_complete = onHeadersComplete,
        };

        s32 parsed = http_parser_execute(&req->parser, &ParserSettings, buf->base, nread);

        if(parsed != nread)
            onError(req, req->parser.status_code);

        free(buf->base);
    }
    else onError(req, 0);
}

static void onHeaderSent(uv_write_t *write, s32 status)
{
    NetRequest* req = write->data;
    http_parser_init(&req->parser, HTTP_RESPONSE);
    req->parser.data = req;

    uv_stream_t* handle = write->handle;
    free(write);

    handle->data = req;
    uv_read_start(handle, allocBuffer, onResponse);
}

static void onConnect(uv_connect_t *con, s32 status)
{
    NetRequest* req = con->data;

    char httpReq[2048];
    snprintf(httpReq, sizeof httpReq, "GET %s HTTP/1.1\nHost: %s\n\n", req->path, req->host);

    uv_buf_t http = uv_buf_init(httpReq, strlen(httpReq));
    uv_write(MOVE((uv_write_t){.data = req}), con->handle, &http, 1, onHeaderSent);

    free(con);
}

static void onResolved(uv_getaddrinfo_t *resolver, s32 status, struct addrinfo *res)
{
    NetRequest* req = resolver->data;

    if (res)
    {
        uv_tcp_connect(MOVE((uv_connect_t){.data = req}), &req->tcp, res->ai_addr, onConnect);
        uv_freeaddrinfo(res);
    }
    else onError(req, 0);

    free(resolver);
}

void tic_net_get(tic_net* net, const char* path, net_get_callback callback, void* calldata)
{
    uv_loop_t* loop = uv_default_loop();

    NetRequest* req = MOVE((NetRequest)
    {
        .host = net->host,
        .callback = callback,
        .calldata = calldata,
        .path = strdup(path),
    });

    uv_tcp_init(loop, &req->tcp);
    uv_getaddrinfo(loop, MOVE((uv_getaddrinfo_t){.data = req}), onResolved, net->host, "80", NULL);
}

void tic_net_start(tic_net *net) {}

void tic_net_end(tic_net *net) 
{
    uv_run(uv_default_loop(), UV_RUN_NOWAIT);
}

tic_net* tic_net_create(const char* host)
{
    tic_net* net = NEW(tic_net);
    memset(net, 0, sizeof(tic_net));

    net->host = host;

    static const char Http[] = "http://";
    if(strstr(host, Http) == host)
        net->host += sizeof Http - 1;

    return net;
}

void tic_net_close(tic_net* net)
{
    free(net);
}

#endif
