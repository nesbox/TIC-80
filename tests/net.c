/* The network layer without a network: a held fake primitive answers the
 * requests, and the get bridge on top of it is what the test checks — a 200
 * is done, any other status is its code, an error body is dropped and a
 * transport error is the code the backend named. The shelf's own tests repeat
 * the 4xx body the primitive delivers.
 *
 * Target net-test in cmake/tests.cmake; `ctest -R net` runs it.
 */
#include "studio/net.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    bool held;
    net_method method;
    const char* url;
    const net_header* headers;
    s32 headersCount;
    const void* body;
    s32 bodySize;
    net_request_callback callback;
    void* calldata;
} Held;

static Held held;

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    assert(!held.held);

    held = (Held){
        .held = true,
        .method = method,
        .url = url,
        .headers = headers,
        .headersCount = headersCount,
        .body = body,
        .bodySize = bodySize,
        .callback = callback,
        .calldata = calldata,
    };
}

static void answerDone(s32 status, const u8* body, s32 size)
{
    assert(held.held);

    held.callback(&(net_request_data){
        .type = net_request_done,
        .done = {.status = status, .size = size, .data = (u8*)body},
        .calldata = held.calldata,
        .url = held.url,
    });

    held.held = false;
}

static void answerError(s32 code)
{
    assert(held.held);

    held.callback(&(net_request_data){
        .type = net_request_error,
        .error = {.code = code},
        .calldata = held.calldata,
        .url = held.url,
    });

    held.held = false;
}

static void answerProgress(s32 size, s32 total)
{
    assert(held.held);

    held.callback(&(net_request_data){
        .type = net_request_progress,
        .progress = {.size = size, .total = total},
        .calldata = held.calldata,
        .url = held.url,
    });
}

typedef struct
{
    s32 progress;
    s32 done;
    s32 error;
    s32 errorCode;
    s32 size;
    u8 body[64];
} Result;

static void onAnswer(const net_get_data* data)
{
    Result* result = data->calldata;

    switch(data->type)
    {
    case net_get_progress:
        result->progress++;
        break;
    case net_get_done:
        result->done++;
        result->size = data->done.size;
        assert(data->done.size <= (s32)sizeof result->body);
        memcpy(result->body, data->done.data, data->done.size);
        break;
    case net_get_error:
        result->error++;
        result->errorCode = data->error.code;
        break;
    }
}

static void testGetIsThePrimitiveGet(void)
{
    Result result = {0};
    tic_net_get(NULL, "/json?fn=version", onAnswer, &result);

    assert(held.held);
    assert(held.method == net_method_get);
    assert(strcmp(held.url, "/json?fn=version") == 0);
    assert(held.headers == NULL);
    assert(held.body == NULL);

    static const u8 body[] = "{\"version\":\"1.5\"}";
    answerDone(200, body, sizeof body - 1);

    assert(result.done == 1 && result.error == 0);
    assert(result.size == (s32)(sizeof body - 1));
    assert(memcmp(result.body, body, sizeof body - 1) == 0);
}

static void testNon200IsItsCode(void)
{
    Result result = {0};
    tic_net_get(NULL, "/missing", onAnswer, &result);

    static const u8 body[] = "{\"error\":\"not found\"}";
    answerDone(404, body, sizeof body - 1);

    assert(result.done == 0);
    assert(result.error == 1 && result.errorCode == 404);
}

static void testTransportErrorIsItsCode(void)
{
    Result result = {0};
    tic_net_get(NULL, "/down", onAnswer, &result);

    answerError(-1);

    assert(result.error == 1 && result.errorCode == -1);
}

static void testProgressPassesThrough(void)
{
    Result result = {0};
    tic_net_get(NULL, "/cart/abc/cart.tic", onAnswer, &result);

    answerProgress(64, 128);
    assert(result.progress == 1);
    assert(held.held);

    static const u8 body[] = "cart";
    answerDone(200, body, sizeof body - 1);
    assert(result.done == 1);
}

int main(void)
{
    testGetIsThePrimitiveGet();
    testNon200IsItsCode();
    testTransportErrorIsItsCode();
    testProgressPassesThrough();

    puts("net ok / 200 done / 4xx code / transport error / progress");
    return 0;
}
