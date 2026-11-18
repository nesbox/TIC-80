// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

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

#include <3ds.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define URL_SIZE 2048

struct tic_net
{
    LightLock tick_lock;
    const char* host;
};

typedef struct
{
    char url[URL_SIZE];

    tic_net* net;
    httpcContext httpc;
    net_request_data data;
    net_request_callback callback;

    net_method method;
    const net_header* headers;
    s32 headersCount;
    const void* body;
    s32 bodySize;

    void* buffer;
    s32 size;
    s32 status;
} net_ctx;

// #define DEBUG

static HTTPC_Method n3ds_method(net_method method)
{
    switch(method)
    {
    case net_method_post: return HTTPC_METHOD_POST;
    case net_method_put: return HTTPC_METHOD_PUT;
    case net_method_delete: return HTTPC_METHOD_DELETE;
    default: return HTTPC_METHOD_GET;
    }
}

static int n3ds_net_setup_context(net_ctx* ctx)
{
    httpcContext* httpc = &ctx->httpc;

    if(httpcOpenContext(httpc, n3ds_method(ctx->method), ctx->url, 1)) return -101;
    if(httpcSetSSLOpt(httpc, SSLCOPT_DisableVerify))
    {
        httpcCloseContext(httpc);
        return -102;
    }
    if(httpcSetKeepAlive(httpc, HTTPC_KEEPALIVE_ENABLED))
    {
        httpcCloseContext(httpc);
        return -103;
    }
    if(httpcAddRequestHeaderField(httpc, "User-Agent", "tic80-n3ds/1.0.0 (httpc)"))
    {
        httpcCloseContext(httpc);
        return -104;
    }
    if(httpcAddRequestHeaderField(httpc, "Connection", "Keep-Alive"))
    {
        httpcCloseContext(httpc);
        return -105;
    }

    for(s32 i = 0; i < ctx->headersCount; i++)
    {
        if(httpcAddRequestHeaderField(httpc, ctx->headers[i].name, ctx->headers[i].value))
        {
            httpcCloseContext(httpc);
            return -106;
        }
    }

    if(ctx->body)
    {
        if(httpcAddPostDataBinary(httpc, ctx->body, ctx->bodySize))
        {
            httpcCloseContext(httpc);
            return -107;
        }
    }

    return 0;
}

#define NET_PAGE_SIZE 4096

static inline bool ctx_resize_buf(net_ctx* ctx, s32 new_size)
{
    // realloc(p, 0) frees the block and answers NULL on newlib: an empty body
    // (a 204, or a 4xx a proxy answers without one) would be a double free and
    // a transport error instead of its status.
    if(new_size <= 0)
    {
        free(ctx->buffer);
        ctx->buffer = NULL;
        ctx->size = 0;
        return true;
    }

    if(ctx->buffer == NULL)
    {
        ctx->buffer = malloc(new_size);
        if(ctx->buffer == NULL)
        {
            return false;
        }
    }
    else if(ctx->size != new_size)
    {
        void* old_buf = ctx->buffer;
        ctx->buffer = realloc(ctx->buffer, new_size);
        if(ctx->buffer == NULL)
        {
            free(old_buf);
            return false;
        }
    }

    ctx->size = new_size;
    return true;
}

// clang-format off
#define NET_EXEC_ERROR_CHECK \
    if (status_code < 0) \
    { \
        printf("net_httpc: error %d\n", status_code); \
        if (ctx->callback != NULL) \
        { \
            ctx->data.type = net_request_error; \
            ctx->data.error.code = status_code; \
            if (!ignore_lock) LightLock_Lock(&ctx->net->tick_lock); \
            ctx->callback(&ctx->data); \
            if (!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock); \
        } \
        httpcCloseContext(&ctx->httpc); \
        if (ctx->buffer != NULL) \
        { \
    		free(ctx->buffer); ctx->size = 0; \
    	} \
        return; \
    }
// clang-format on

static void n3ds_net_execute(net_ctx* ctx, bool ignore_lock)
{
    bool redirecting = true;
    s32 status_code = -1;

    ctx->data.url = ctx->url;
    while(redirecting)
    {
#ifdef DEBUG
        printf("url: %s\n", ctx->url);
#endif
        redirecting = false;

        status_code = n3ds_net_setup_context(ctx);
        if(status_code < 0)
        {
            break;
        }

        if(httpcBeginRequest(&ctx->httpc))
        {
            status_code = -2;
            break;
        }

        // libctru hands the code back in a u32, the error codes above are not
        u32 code = 0;
        if(httpcGetResponseStatusCode(&ctx->httpc, &code))
        {
            status_code = -3;
            break;
        }
        status_code = (s32)code;

        if((status_code >= 301 && status_code <= 303) || (status_code >= 307 && status_code <= 308))
        {
            if(httpcGetResponseHeader(&ctx->httpc, "Location", ctx->url, URL_SIZE - 1))
            {
                status_code = -4;
                break;
            }

            redirecting = true;
            httpcCloseContext(&ctx->httpc);
        }
    }

    NET_EXEC_ERROR_CHECK;

    ctx->status = status_code;

    u32 state = HTTPC_RESULTCODE_DOWNLOADPENDING;
    u32 read_size = 0;
    while(state == HTTPC_RESULTCODE_DOWNLOADPENDING)
    {
        s32 old_size = ctx->size;
        if(!ctx_resize_buf(ctx, ctx->size + NET_PAGE_SIZE))
        {
            httpcCloseContext(&ctx->httpc);
            status_code = -5;
            break;
        }
        u8* old_ptr = ((u8*)ctx->buffer) + old_size;
        state = httpcDownloadData(&ctx->httpc, old_ptr, NET_PAGE_SIZE, &read_size);
        if(state == HTTPC_RESULTCODE_DOWNLOADPENDING || state == 0)
        {
            ctx_resize_buf(ctx, old_size + (s32)read_size);
            if(ctx->callback != NULL)
            {
                if(ignore_lock || !LightLock_TryLock(&ctx->net->tick_lock))
                {
                    ctx->data.type = net_request_progress;
                    u32 size = 0, total = 0;
                    if(!httpcGetDownloadSizeState(&ctx->httpc, &size, &total))
                    {
                        ctx->data.progress.size = (s32)size;
                        ctx->data.progress.total = (s32)total;

                        if(ctx->data.progress.total < ctx->data.progress.size)
                        {
                            ctx->data.progress.total = ctx->data.progress.size;
                        }
                        ctx->callback(&ctx->data);
                    }
                    if(!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock);
                }
            }
        }
    }

#ifdef DEBUG
    printf("downloaded: %d bytes\n", ctx->size);
#endif

    if(state != 0)
    {
        status_code = -6;
    }
    NET_EXEC_ERROR_CHECK;

    if(ctx->callback != NULL)
    {
        ctx->data.type = net_request_done;
        ctx->data.done.status = ctx->status;
        ctx->data.done.data = ctx->buffer;
        ctx->data.done.size = ctx->size;
        if(!ignore_lock) LightLock_Lock(&ctx->net->tick_lock);
        ctx->callback(&ctx->data);
        if(!ignore_lock) LightLock_Unlock(&ctx->net->tick_lock);
    }
    httpcCloseContext(&ctx->httpc);
}

static void n3ds_net_init(tic_net* net)
{
    httpcInit(0);

    memset(net, 0, sizeof(tic_net));
    LightLock_Init(&net->tick_lock);
}

static void n3ds_net_free(tic_net* net)
{
    (void)net;
    httpcExit();
}

static void n3ds_net_get_thread(net_ctx* ctx)
{
    n3ds_net_execute(ctx, false);

    if(ctx->buffer != NULL)
    {
        free(ctx->buffer);
    }
    free(ctx);
}

static void n3ds_net_apply_url(net_ctx* ctx, const char* url)
{
    snprintf(ctx->url, URL_SIZE, "http://%s%s", ctx->net->host, url);
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

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    net_ctx ctx =
        {
            .net = net,
            .callback = callback,
            .data = {.calldata = calldata},
            .method = method,
            .headers = headers,
            .headersCount = headersCount,
            .body = body,
            .bodySize = bodySize,
        };

    n3ds_net_apply_url(&ctx, url);

    s32 priority;
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    threadCreate((ThreadFunc)n3ds_net_get_thread, MOVE(ctx), 16 * 1024, priority - 1, -1, true);
}

void tic_net_close(tic_net* net)
{
    n3ds_net_free(net);
    free(net);
}

void tic_net_start(tic_net* net)
{
    LightLock_Lock(&net->tick_lock);
}

void tic_net_end(tic_net* net)
{
    LightLock_Unlock(&net->tick_lock);
}
