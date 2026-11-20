/* The account without the studio: the token file under an unversioned path,
 * the pairing state machine over a held fake network, logout's revoke and the
 * 401 path. The filesystem is a real one on a temporary root; the network is
 * the test's own and no socket is opened.
 *
 * Target account-test in cmake/tests.cmake; `ctest -R account` runs it.
 */
#include "studio/account.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/paths.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The platform the filesystem reaches for; the test opens nothing.
void tic_sys_open_path(const char* path)
{
    (void)path;
}

// The held request: one at a time, as the account sends them.
typedef struct
{
    bool held;
    net_method method;
    char url[256];
    net_header headers[4];
    s32 headersCount;
    char body[512];
    net_request_callback callback;
    void* calldata;
} Held;

static Held held;

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    assert(!held.held);
    assert(headersCount <= (s32)(sizeof held.headers / sizeof *held.headers));

    memset(&held, 0, sizeof held);
    held.held = true;
    held.method = method;
    snprintf(held.url, sizeof held.url, "%s", url);
    held.headersCount = headersCount;
    memcpy(held.headers, headers, headersCount * sizeof *headers);
    if(body && bodySize > 0)
    {
        assert(bodySize < (s32)sizeof held.body);
        memcpy(held.body, body, bodySize);
    }
    held.callback = callback;
    held.calldata = calldata;
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

static void answerDone(s32 status, const char* json)
{
    assert(held.held);

    net_request_callback callback = held.callback;
    void* calldata = held.calldata;
    held.held = false;

    callback(&(net_request_data){
        .type = net_request_done,
        .done = {.status = status, .size = (s32)strlen(json), .data = (u8*)json},
        .calldata = calldata,
        .url = held.url,
    });
}

static void answerError(s32 code)
{
    assert(held.held);

    net_request_callback callback = held.callback;
    void* calldata = held.calldata;
    held.held = false;

    callback(&(net_request_data){
        .type = net_request_error,
        .error = {.code = code},
        .calldata = calldata,
        .url = held.url,
    });
}

typedef struct
{
    s32 calls;
    account_pairing_state state;
    char text[512];
} Capture;

static void onPairing(account_pairing_state state, const char* text, void* data)
{
    Capture* capture = data;
    capture->calls++;
    capture->state = state;
    snprintf(capture->text, sizeof capture->text, "%s", text ? text : "");
}

// startPairing drives a login through the start request and its code answer.
static void startPairing(tic_account* account, Capture* capture)
{
    assert(tic_account_login(account, onPairing, capture));
    assert(held.held);
    assert(held.method == net_method_post);
    assert(strcmp(held.url, "/api/v1/client/device") == 0);
    assert(strstr(held.body, "name=") == held.body);

    // A progress tick is not the pairing's answer.
    answerProgress(10, 100);
    assert(held.held);
    assert(tic_account_login_active(account));
    assert(capture->calls == 0);

    answerDone(201,
               "{\"device_code\":\"0123456789abcdef\","
               "\"user_code\":\"ABCD-EFGH\","
               "\"verification_uri\":\"https://dev.tic80.com/link\","
               "\"expires_in\":600,\"interval\":1}");

    assert(capture->calls == 1);
    assert(capture->state == account_pairing_code);
    assert(strstr(capture->text, "https://dev.tic80.com/link") != NULL);
    assert(strstr(capture->text, "ABCD-EFGH") != NULL);
}

static void testTokenRoundTrip(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    assert(account);
    assert(!tic_account_signedin(account));

    Capture capture = {0};
    startPairing(account, &capture);

    // the first poll is due after the server's interval
    tic_account_update(account, 500);
    assert(!held.held);
    tic_account_update(account, 500);
    assert(held.held);
    assert(held.method == net_method_get);
    assert(strcmp(held.url, "/api/v1/client/device/0123456789abcdef") == 0);

    answerProgress(10, 100);
    assert(held.held);
    assert(tic_account_login_active(account));

    answerDone(200, "{\"status\":\"pending\"}");

    tic_account_update(account, 1000);
    assert(held.held);
    answerDone(200,
               "{\"status\":\"approved\","
               "\"token\":\"a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90\","
               "\"user\":\"member\"}");

    assert(capture.calls == 2);
    assert(capture.state == account_pairing_approved);
    assert(strcmp(capture.text, "member") == 0);
    assert(tic_account_signedin(account));
    assert(strcmp(tic_account_user(account), "member") == 0);
    assert(strlen(tic_account_token(account)) == 64);

    // the filesystem sees the same member
    assert(tic_fs_signedin(fs));

    // the file is at the unversioned path, and a new build finds it
    s32 size = 0;
    void* file = tic_fs_loadroot(fs, TIC_ACCOUNT_PATH, &size);
    assert(file);
    free(file);

    // the path has no version segment: a build with another versioned
    // directory still reads the same file
    void* versioned = tic_fs_loadroot(fs, TIC_LOCAL_VERSION "account.json", &size);
    assert(!versioned);

    tic_account_close(account);

    tic_account* next = tic_account_create(fs, NULL);
    assert(tic_account_signedin(next));
    assert(strcmp(tic_account_user(next), "member") == 0);
    assert(strcmp(tic_account_token(next), "a1b2c3d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90") == 0);
    assert(!held.held); // no request to load it

    // logout clears the local token at once and asks the server to revoke it
    char token[96];
    snprintf(token, sizeof token, "%s", tic_account_token(next));
    tic_account_logout(next);
    assert(!tic_account_signedin(next));
    assert(!tic_fs_signedin(fs));
    assert(held.held);
    assert(held.method == net_method_delete);
    assert(strcmp(held.url, "/api/v1/client/token") == 0);
    assert(held.headersCount == 1);
    assert(strcmp(held.headers[0].name, "Authorization") == 0);
    char bearer[128];
    snprintf(bearer, sizeof bearer, "Bearer %s", token);
    assert(strcmp(held.headers[0].value, bearer) == 0);

    answerProgress(10, 100);
    assert(held.held);

    answerDone(200, "{\"ok\":true}");

    tic_account_close(next);
    tic_account* third = tic_account_create(fs, NULL);
    assert(!tic_account_signedin(third));
    tic_account_close(third);
}

static void testDenied(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    startPairing(account, &capture);
    tic_account_update(account, 1000);
    assert(held.held);
    answerDone(200, "{\"status\":\"denied\"}");

    assert(capture.state == account_pairing_denied);
    assert(!tic_account_signedin(account));
    tic_account_close(account);
}

static void testExpired(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    startPairing(account, &capture);
    tic_account_update(account, 1000);
    answerDone(404, "{\"error\":\"unknown device code\"}");

    assert(capture.state == account_pairing_expired);
    assert(!tic_account_signedin(account));
    tic_account_close(account);
}

static void testTransportError(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    assert(tic_account_login(account, onPairing, &capture));
    answerError(-1);

    assert(capture.calls == 1);
    assert(capture.state == account_pairing_error);
    assert(capture.text[0] == '\0');
    assert(!tic_account_signedin(account));

    // and inside the poll
    Capture again = {0};
    startPairing(account, &again);
    tic_account_update(account, 1000);
    answerError(-1);
    assert(again.state == account_pairing_error);
    assert(!tic_account_signedin(account));

    tic_account_close(account);
}

static void testCancel(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    startPairing(account, &capture);
    tic_account_update(account, 1000);
    assert(held.held);

    tic_account_cancel_login(account);
    // the in-flight answer arrives after the cancel and changes nothing
    answerDone(200, "{\"status\":\"approved\",\"token\":\"t\",\"user\":\"late\"}");
    assert(capture.calls == 1);
    assert(!tic_account_signedin(account));

    // no poll follows either
    tic_account_update(account, 5000);
    assert(!held.held);

    tic_account_close(account);
}

static void testUnauthorizedClearsTheToken(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    startPairing(account, &capture);
    tic_account_update(account, 1000);
    answerDone(200, "{\"status\":\"approved\",\"token\":\"t\",\"user\":\"member\"}");
    assert(tic_account_signedin(account));

    tic_account_unauthorized(account);
    assert(!tic_account_signedin(account));
    assert(!tic_fs_signedin(fs));
    assert(!held.held); // the 401 path does not call the server

    tic_account_close(account);
}

static void testServerErrorCarriesItsMessage(tic_fs* fs)
{
    tic_account* account = tic_account_create(fs, NULL);
    Capture capture = {0};

    assert(tic_account_login(account, onPairing, &capture));
    answerDone(503, "{\"error\":\"pairing is briefly unavailable\"}");

    assert(capture.calls == 1);
    assert(capture.state == account_pairing_error);
    assert(strcmp(capture.text, "pairing is briefly unavailable") == 0);

    tic_account_close(account);
}

int main(void)
{
    char root[] = "/tmp/tic80-account-XXXXXX";
    assert(mkdtemp(root));

    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);
    assert(!tic_fs_makedir(fs, TIC_LOCAL));

    testTokenRoundTrip(fs);
    testDenied(fs);
    testExpired(fs);
    testTransportError(fs);
    testCancel(fs);
    testUnauthorizedClearsTheToken(fs);
    testServerErrorCarriesItsMessage(fs);

    tic_fs_delfile(fs, TIC_ACCOUNT_PATH);
    tic_fs_deldir(fs, TIC_LOCAL);
    tic_fs_deldir(fs, "");
    free(fs);

    puts("account ok / token round-trip / denied / expired / transport / cancel / 401 / server error");
    return 0;
}
