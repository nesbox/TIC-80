/* The desktop transport against a real local HTTP server: the naett backend
 * sends a POST with a header and a body, and the answer's status and body reach
 * the caller — a 200 and a 409 alike. A request to a closed port is the
 * transport error. The bridge's translation is net-test's; this is the wire.
 *
 * Target net-naett-test in cmake/tests.cmake; `ctest -R net-naett` runs it.
 */
#include "studio/net.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct
{
    int listener;
    pthread_t thread;
    bool started;

    // what the server received
    char method[16];
    char path[256];
    char authorization[256];
    char body[1024];
    s32 bodySize;

    // what it answers
    int status;
    const char* response;
} Server;

static Server server;

static void* serveOnce(void* arg)
{
    (void)arg;

    int fd = accept(server.listener, NULL, NULL);
    assert(fd >= 0);

    char request[8192];
    s32 size = 0;
    while(size < (s32)sizeof request - 1)
    {
        ssize_t got = recv(fd, request + size, sizeof request - 1 - size, 0);
        if(got <= 0)
            break;
        size += (s32)got;
        request[size] = '\0';
        if(strstr(request, "\r\n\r\n"))
            break;
    }
    request[size] = '\0';

    sscanf(request, "%15s %255s", server.method, server.path);

    // the Content-Length and the one header the test cares about
    s32 contentLength = 0;
    const char* header = request;
    while((header = strstr(header, "\r\n")) != NULL)
    {
        header += 2;
        if(!strncasecmp(header, "Content-Length:", 15))
            contentLength = atoi(header + 15);
        if(!strncasecmp(header, "Authorization:", 14))
        {
            const char* value = header + 14;
            while(*value == ' ')
                value++;
            s32 len = 0;
            while(value[len] && value[len] != '\r' && len < (s32)sizeof server.authorization - 1)
                len++;
            memcpy(server.authorization, value, len);
            server.authorization[len] = '\0';
        }
        if(!strncmp(header, "\r\n", 2))
            break;
    }

    const char* body = strstr(request, "\r\n\r\n");
    if(body && contentLength > 0)
    {
        body += 4;
        s32 have = size - (s32)(body - request);

        // The body may arrive after the headers did: keep reading it.
        while(have < contentLength && size < (s32)sizeof request - 1)
        {
            ssize_t got = recv(fd, request + size, sizeof request - 1 - size, 0);
            if(got <= 0)
                break;
            size += (s32)got;
            request[size] = '\0';
            have = size - (s32)(body - request);
        }

        server.bodySize = contentLength < (s32)sizeof server.body ? contentLength : (s32)sizeof server.body - 1;
        if(have < server.bodySize)
            server.bodySize = have > 0 ? have : 0;
        memcpy(server.body, body, server.bodySize);
        server.body[server.bodySize] = '\0';
    }

    char response[1024];
    snprintf(response, sizeof response,
             "HTTP/1.1 %d X\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
             server.status, (int)strlen(server.response), server.response);
    send(fd, response, strlen(response), 0);
    close(fd);

    return NULL;
}

static int startServer(int status, const char* response)
{
    memset(&server, 0, sizeof server);
    server.status = status;
    server.response = response;

    server.listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(server.listener >= 0);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    assert(bind(server.listener, (struct sockaddr*)&addr, sizeof addr) == 0);
    assert(listen(server.listener, 1) == 0);

    socklen_t len = sizeof addr;
    assert(getsockname(server.listener, (struct sockaddr*)&addr, &len) == 0);

    assert(pthread_create(&server.thread, NULL, serveOnce, NULL) == 0);
    server.started = true;

    return ntohs(addr.sin_port);
}

static void stopServer(void)
{
    if(server.started)
    {
        pthread_join(server.thread, NULL);
        server.started = false;
    }
    close(server.listener);
}

typedef struct
{
    s32 done;
    s32 error;
    s32 errorCode;
    s32 status;
    s32 size;
    u8 body[1024];
} Result;

static void onAnswer(const net_request_data* data)
{
    Result* result = data->calldata;

    switch(data->type)
    {
        case net_request_done:
            result->done++;
            result->status = data->done.status;
            result->size = data->done.size;
            assert(data->done.size <= (s32)sizeof result->body);
            memcpy(result->body, data->done.data, data->done.size);
            break;
        case net_request_error:
            result->error++;
            result->errorCode = data->error.code;
            break;
        default:
            break;
    }
}

typedef struct
{
    s32 done;
    char body[64];
} BridgeResult;

static void onBridge(const net_get_data* answer)
{
    BridgeResult* result = answer->calldata;

    if(answer->type == net_get_done)
    {
        result->done++;
        assert(answer->done.size < (s32)sizeof result->body);
        memcpy(result->body, answer->done.data, answer->done.size);
        result->body[answer->done.size] = '\0';
    }
}

// The transport completes on tic_net_end, which the studio calls every tick.
static void waitDone(tic_net* net, Result* result)
{
    for(int i = 0; i < 500 && result->done == 0 && result->error == 0; i++)
    {
        tic_net_end(net);
        usleep(10 * 1000);
    }
    assert(result->done > 0 || result->error > 0);
}

static void testPostCarriesHeaderAndBody(void)
{
    int port = startServer(201, "{\"key\":\"ok\"}");

    char host[64];
    snprintf(host, sizeof host, "http://127.0.0.1:%d", port);
    tic_net* net = tic_net_create(host);
    assert(net);

    static const u8 payload[] = "cart-bytes";
    const net_header headers[] = {{"Authorization", "Bearer token123"}};
    Result result = {0};

    tic_net_request(net, net_method_post, "/api/v1/client/device", headers, 1,
                    payload, sizeof payload - 1, onAnswer, &result);
    waitDone(net, &result);
    stopServer();

    assert(strcmp(server.method, "POST") == 0);
    assert(strcmp(server.path, "/api/v1/client/device") == 0);
    assert(strcmp(server.authorization, "Bearer token123") == 0);
    assert(server.bodySize == (s32)(sizeof payload - 1));
    assert(memcmp(server.body, payload, sizeof payload - 1) == 0);

    // a 201 is a completed answer with its body
    assert(result.done == 1 && result.error == 0);
    assert(result.status == 201);
    assert(result.size == 12 && memcmp(result.body, "{\"key\":\"ok\"}", 12) == 0);

    tic_net_close(net);
}

static void testErrorStatusCarriesItsBody(void)
{
    int port = startServer(409, "{\"error\":\"Your shelf holds 100 carts\"}");

    char host[64];
    snprintf(host, sizeof host, "http://127.0.0.1:%d", port);
    tic_net* net = tic_net_create(host);
    assert(net);

    Result result = {0};
    tic_net_request(net, net_method_put, "/api/v1/client/shelf/x", NULL, 0,
                    "x", 1, onAnswer, &result);
    waitDone(net, &result);
    stopServer();

    assert(strcmp(server.method, "PUT") == 0);
    assert(result.done == 1 && result.error == 0);
    assert(result.status == 409);
    assert(strstr((const char*)result.body, "100 carts") != NULL);

    tic_net_close(net);
}

static void testDeleteHasNoBody(void)
{
    int port = startServer(200, "{\"count\":0}");

    char host[64];
    snprintf(host, sizeof host, "http://127.0.0.1:%d", port);
    tic_net* net = tic_net_create(host);
    assert(net);

    const net_header headers[] = {{"Authorization", "Bearer t"}};
    Result result = {0};
    tic_net_request(net, net_method_delete, "/api/v1/client/shelf/x", headers, 1,
                    NULL, 0, onAnswer, &result);
    waitDone(net, &result);
    stopServer();

    assert(strcmp(server.method, "DELETE") == 0);
    assert(server.bodySize == 0);
    assert(result.done == 1 && result.status == 200);

    tic_net_close(net);
}

static void testATransportFailureIsAnError(void)
{
    // bind and close, so nothing listens on the port any more
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    assert(bind(probe, (struct sockaddr*)&addr, sizeof addr) == 0);
    socklen_t len = sizeof addr;
    assert(getsockname(probe, (struct sockaddr*)&addr, &len) == 0);
    close(probe);

    char host[64];
    snprintf(host, sizeof host, "http://127.0.0.1:%d", ntohs(addr.sin_port));
    tic_net* net = tic_net_create(host);
    assert(net);

    Result result = {0};
    tic_net_request(net, net_method_get, "/x", NULL, 0, NULL, 0, onAnswer, &result);
    waitDone(net, &result);

    assert(result.done == 0 && result.error == 1);
    assert(result.errorCode <= 0);

    tic_net_close(net);
}

static void testGetBridgeOverTheWire(void)
{
    int port = startServer(200, "version=1.5");

    char host[64];
    snprintf(host, sizeof host, "http://127.0.0.1:%d", port);
    tic_net* net = tic_net_create(host);
    assert(net);

    BridgeResult result = {0};
    tic_net_get(net, "/api?fn=version", onBridge, &result);
    for(int i = 0; i < 500 && result.done == 0; i++)
    {
        tic_net_end(net);
        usleep(10 * 1000);
    }
    stopServer();

    assert(result.done == 1);
    assert(strcmp(result.body, "version=1.5") == 0);

    tic_net_close(net);
}

int main(void)
{
    testPostCarriesHeaderAndBody();
    testErrorStatusCarriesItsBody();
    testDeleteHasNoBody();
    testATransportFailureIsAnError();
    testGetBridgeOverTheWire();

    puts("naett ok / POST header+body / 409 body / DELETE / transport error / GET bridge");
    return 0;
}
