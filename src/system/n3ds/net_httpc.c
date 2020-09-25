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

#include <stdio.h>
#include <stdlib.h>
#include "system.h"
#include "net_httpc.h"

typedef struct {
	char url[TICNAME_MAX];

	tic_n3ds_net *net;
	httpcContext httpc;
	HttpGetData data;
	HttpGetCallback callback;

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
			ctx->data.type = HttpGetError; \
			ctx->data.error.code = status_code; \
			if (!ignore_lock) LightLock_Lock(ctx->net->tick_lock); \
			ctx->callback(&ctx->data); \
			if (!ignore_lock) LightLock_Unlock(ctx->net->tick_lock); \
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
			if (httpcGetResponseHeader(&ctx->httpc, "Location", ctx->url, TICNAME_MAX)) {
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
				if (ignore_lock || !LightLock_TryLock(ctx->net->tick_lock)) {
					ctx->data.type = HttpGetProgress;
					if (!httpcGetDownloadSizeState(&ctx->httpc, &ctx->data.progress.size, &ctx->data.progress.total)) {
						if (ctx->data.progress.total < ctx->data.progress.size) {
							ctx->data.progress.total = ctx->data.progress.size;
						}
						ctx->callback(&ctx->data);
					}
					if (!ignore_lock) LightLock_Unlock(ctx->net->tick_lock);
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
		ctx->data.type = HttpGetDone;
		ctx->data.done.data = ctx->buffer;
		ctx->data.done.size = ctx->size;
		if (!ignore_lock) LightLock_Lock(ctx->net->tick_lock);
		ctx->callback(&ctx->data);
		if (!ignore_lock) LightLock_Unlock(ctx->net->tick_lock);
	}
	httpcCloseContext(&ctx->httpc);
}

void n3ds_net_init(tic_n3ds_net *net, LightLock *tick_lock) {
	httpcInit(0);

	memset(net, 0, sizeof(tic_n3ds_net));
	net->tick_lock = tick_lock;
}

void n3ds_net_free(tic_n3ds_net *net) {
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
	strncpy(ctx->url, TIC_WEBSITE, TICNAME_MAX);
	strncat(ctx->url, url, TICNAME_MAX - 1);
}

void n3ds_net_get(tic_n3ds_net *net, const char *url, HttpGetCallback callback, void *calldata) {
	s32 priority;
	net_ctx *ctx;

	ctx = malloc(sizeof(net_ctx));
	memset(&ctx, 0, sizeof(net_ctx));

	n3ds_net_apply_url(ctx, url);
	ctx->net = net;
	ctx->callback = callback;
	ctx->data.calldata = calldata;

	svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
	threadCreate((ThreadFunc) n3ds_net_get_thread, ctx, 16 * 1024, priority - 1, -1, true);
}

void* n3ds_net_get_sync(tic_n3ds_net *net, const char *url, s32 *size) {
	net_ctx ctx;
	memset(&ctx, 0, sizeof(net_ctx));

	n3ds_net_apply_url(&ctx, url);
	ctx.net = net;
	n3ds_net_execute(&ctx, true);

	if (size != NULL) {
		*size = ctx.size;
	}
	return ctx.buffer;
}