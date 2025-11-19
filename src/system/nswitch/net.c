// MIT License

// Copyright (c) 2025 Carsten Teibes

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
#include "version.h"

#include <switch.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#define STR(s) TO_STRING(s)
#define TO_STRING(s) #s
#define TIC_VERSION_STRING STR(TIC_VERSION_MAJOR) "." STR(TIC_VERSION_MINOR) "." STR(TIC_VERSION_REVISION)

#define URL_SIZE 2048

typedef enum {
    RS_RUNNING = 0,
    RS_OK,
    RS_ERROR,
} req_state;

typedef struct {
    tic_net *net;

    char url[URL_SIZE];
    volatile req_state state;

    // for callback
    net_get_callback callback;
    void *calldata;

    // for progress function
    curl_off_t lastruntime;
    CURL *curl;

    // for write function
    char *buffer;
    size_t size;
} net_ctx;

struct tic_net {
    char host[URL_SIZE];
    Mutex lock;
    Thread thread;
    UEvent poll_event, exit_event;
    CURLM *curl_multi;
    bool close_sockets;

    net_ctx **requests;
    size_t count;
};

static size_t switch_write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    net_ctx *ctx = (net_ctx *)userp;

    size_t realsize = size * nmemb;
    size_t old_size = ctx->size;

    ctx->buffer = realloc(ctx->buffer, ctx->size + realsize + 1);
    if(!ctx->buffer) {
        printf("out of memory!\n");
        return 0; // signal error
    }

    // append data
    memcpy(&(ctx->buffer[old_size]), contents, realsize);
    ctx->size += realsize;
    ctx->buffer[ctx->size] = '\0';

    return realsize;
}

static int switch_xferinfo_cb(void *userp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow) {
    net_ctx *ctx = (net_ctx *)userp;

    // no size yet
    if (!dltotal) return 0;

    curl_off_t curtime;
    curl_easy_getinfo(ctx->curl, CURLINFO_TOTAL_TIME_T, &curtime);
    if((curtime - ctx->lastruntime) >= 100000U) { // every 100ms
        ctx->lastruntime = curtime;
        if (ctx->callback != NULL) {
            if (mutexTryLock(&ctx->net->lock)) {
                // send progress
                net_get_data getData = {
                    .type = net_get_progress,
                    .progress = {
                        .size = dlnow,
                        .total = dltotal,
                    },
                    .calldata = ctx->calldata,
                    .url = ctx->url,
                };
                ctx->callback(&getData);
                mutexUnlock(&ctx->net->lock);
            }
        }
    }

    return 0;
}

static void switch_worker_thread(void *userp) {
    tic_net* net = (tic_net *)userp;

    Result rc;
    int idx;
    while(1) {
        // wait until something to do
        rc = waitMulti(&idx, -1, waiterForUEvent(&net->poll_event), waiterForUEvent(&net->exit_event));
        if(R_FAILED(rc)) {
            continue;
        }

        if(idx == 1) { // exit_event
            break;
        }

        ueventClear(&net->poll_event);

        int still_running = 1;
        while(still_running) {
            CURLMcode mc = curl_multi_perform(net->curl_multi, &still_running);

            if(still_running) {
                mc = curl_multi_poll(net->curl_multi, NULL, 0, 1000, NULL);
                if(mc != CURLM_OK) {
                    printf("curl_multi failed, code %d.\n", mc);
                    break;
                }
            }

            CURLMsg *msg;
            int msgs_left;
            while((msg = curl_multi_info_read(net->curl_multi, &msgs_left)) != NULL) {
                if(msg->msg == CURLMSG_DONE) { // request done
                    CURL *curl = msg->easy_handle;

                    net_ctx *ctx = NULL;
                    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &ctx);
                    if(ctx) {
                        if(msg->data.result == CURLE_OK) { // all ok
                            ctx->state = RS_OK;
                        } else { // some error happened
                            // TODO: maybe publish msg->data.result
                            ctx->state = RS_ERROR;
                        }
                    }

                    curl_multi_remove_handle(net->curl_multi, curl);
                    curl_easy_cleanup(curl);
                }
            }
        }
    }
}

tic_net* tic_net_create(const char* host) {
    tic_net* net = NEW(tic_net);
    memset(net, 0, sizeof(tic_net));

    strncpy(net->host, host, URL_SIZE - 1);
    net->host[URL_SIZE - 1] = '\0';

    mutexInit(&net->lock);
    ueventCreate(&net->poll_event, false);
    ueventCreate(&net->exit_event, false);

    Result rc = socketInitializeDefault();
    if (R_SUCCEEDED(rc)) {
        net->close_sockets = true;
    } else {
        if(R_VALUE(rc) == MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized)) {
            net->close_sockets = false; // we use nxlink currently
        } else {
            printf("socket init fail!\n");
            return NULL;
        }
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    net->curl_multi = curl_multi_init();
    if(!net->curl_multi) {
        printf("curl init fail!\n");
        return NULL;
    }
    curl_multi_setopt(net->curl_multi, CURLMOPT_MAXCONNECTS, 8L);

    rc = threadCreate(&net->thread, switch_worker_thread, (void*)net, NULL, 0x10000, 0x2B, -2);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&net->thread);
        if (R_FAILED(rc)) {
            printf("thread init fail!\n");
            return NULL;
        }
    }

    return net;
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata) {
    net_ctx *ctx = NEW(net_ctx);
    memset(ctx, 0, sizeof(*ctx));

    ctx->net = net;

    snprintf(ctx->url, URL_SIZE - 1, "%s%s", net->host, url);
    ctx->state = RS_RUNNING;

    ctx->callback = callback;
    ctx->calldata = calldata;

    ctx->lastruntime = 0;

    ctx->buffer = NULL;
    ctx->size = 0;

    // queue transfer
    ctx->curl = curl_easy_init();
    if (ctx->curl) {
        curl_easy_setopt(ctx->curl, CURLOPT_URL, ctx->url);
        curl_easy_setopt(ctx->curl, CURLOPT_PRIVATE, ctx);
        curl_easy_setopt(ctx->curl, CURLOPT_USERAGENT, "tic-80-switch/" TIC_VERSION_STRING);
        curl_easy_setopt(ctx->curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L); // 3s
        curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, 1L); // CURLFOLLOW_ALL
        curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, switch_write_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx);
        curl_easy_setopt(ctx->curl, CURLOPT_XFERINFOFUNCTION, switch_xferinfo_cb);
        curl_easy_setopt(ctx->curl, CURLOPT_XFERINFODATA, ctx);
        curl_easy_setopt(ctx->curl, CURLOPT_NOPROGRESS, 0L);
        //curl_easy_setopt(ctx->curl, CURLOPT_VERBOSE, 1L);
        curl_multi_add_handle(net->curl_multi, ctx->curl);
        ueventSignal(&net->poll_event);
    }

    // add request to list
    net->requests = realloc(net->requests, sizeof(*net->requests) * (net->count + 1));
    net->requests[net->count++] = ctx;
}

void tic_net_close(tic_net* net) {
    // close thread
    ueventSignal(&net->exit_event);
    curl_multi_wakeup(net->curl_multi);
    threadWaitForExit(&net->thread);
    threadClose(&net->thread);

    curl_multi_cleanup(net->curl_multi);
    curl_global_cleanup();

    for(size_t i = 0; i < net->count; i++) {
        net_ctx *ctx = net->requests[i];
        if(ctx) {
            // reset buffer
            if(ctx->buffer != NULL) {
                free(ctx->buffer);
                ctx->buffer = NULL;
                ctx->size = 0;
            }
            free(ctx);
        }
    }
    if(net->requests)
        free(net->requests);

    if(net->close_sockets) {
        socketExit();
    }
    free(net);
}

void tic_net_start(tic_net *net) {
    mutexLock(&net->lock);
}

void tic_net_end(tic_net *net) {
    if(!net->requests) {
        mutexUnlock(&net->lock);
        return;
    }

    // handle finished requests
    for(size_t i = 0; i < net->count; i++) {
        net_ctx *ctx = net->requests[i];
        if(ctx && ctx->state != RS_RUNNING) {
            if(ctx->callback != NULL) {
                net_get_data getData = {
                    .calldata = ctx->calldata,
                    .url = ctx->url,
                };

                if(ctx->state == RS_OK) {
                    getData.type = net_get_done;
                    getData.done.data = ctx->buffer;
                    getData.done.size = ctx->size;
                } else {
                    getData.type = net_get_error;
                    getData.error.code = -1;
                }
                ctx->callback(&getData);
            }

            // reset buffer
            if(ctx->buffer != NULL) {
                free(ctx->buffer);
                ctx->buffer = NULL;
                ctx->size = 0;
            }

            // delete context
            free(net->requests[i]);
            net->requests[i] = NULL;
        }
    }

    mutexUnlock(&net->lock);
}
