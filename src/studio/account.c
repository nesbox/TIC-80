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

#include "studio/paths.h"
#include "studio/system.h"
#include "account.h"
#include "fs.h"
#include "net.h"
#include "ext/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_SIZE  96
#define CODE_SIZE   32
#define URI_SIZE    256
#define NAME_SIZE   TICNAME_MAX

// The pairing is the server's (client-auth spec): a start returns a device
// code, a user code and the URL to show; the poll repeats at the server's
// interval until it answers approved, denied or unknown.
typedef enum
{
    PairingIdle,
    PairingWaiting,
} PairingState;

struct tic_account
{
    tic_fs* fs;
    tic_net* net;

    char token[TOKEN_SIZE];
    char user[NAME_SIZE];

    PairingState pairing;
    bool inFlight;
    u32 waited;
    u32 intervalMs;
    u32 generation;

    char deviceCode[TOKEN_SIZE];
    char userCode[CODE_SIZE];
    char uri[URI_SIZE];

    account_pairing_callback callback;
    void* data;
};

// One request's context: the callback outlives the call, so the pairing
// generation is kept with it and a stale answer after logout or cancel is
// dropped.
typedef struct
{
    tic_account* account;
    u32 generation;
} AccountOp;

// The start's request also owns its header and body, which must stay valid
// until the answer arrives. The operation is its first member, so the callbacks
// can free the whole allocation through it.
typedef struct
{
    AccountOp op;
    net_header header;
    char body[512];
} StartOp;

static const char* platformName(void)
{
#if defined(__3DS__)
    return "Nintendo 3DS";
#elif defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__TANMATSU__)
    return "Tanmatsu";
#elif defined(__TIC_WINDOWS__)
    return "Windows";
#elif defined(__TIC_MACOSX__)
    return "macOS";
#elif defined(__TIC_LINUX__)
    return "Linux";
#else
    return "TIC-80";
#endif
}

// The device label the approval page shows: the build and the platform, under
// the server's 64-character cap.
static void deviceLabel(char* out, s32 size)
{
    snprintf(out, size, "%s %s (%s)", TIC_NAME, TIC_VERSION, platformName());
}

// urlEncode keeps [A-Za-z0-9_.~-] and escapes the rest; the label has spaces
// and parentheses, which a form body must not carry raw.
static void urlEncode(char* out, s32 size, const char* in)
{
    static const char Hex[] = "0123456789ABCDEF";
    s32 o = 0;

    for(const char* p = in; *p && o < size - 3; p++)
    {
        u8 c = (u8)*p;

        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~')
        {
            out[o++] = (char)c;
        }
        else
        {
            out[o++] = '%';
            out[o++] = Hex[c >> 4];
            out[o++] = Hex[c & 0xf];
        }
    }

    out[o] = '\0';
}

bool tic_account_signedin(tic_account* account)
{
    return account->token[0] != '\0';
}

const char* tic_account_user(tic_account* account)
{
    return account->token[0] ? account->user : "";
}

const char* tic_account_token(tic_account* account)
{
    return account->token[0] ? account->token : "";
}

static bool remoteSignedIn(void* data)
{
    return tic_account_signedin(data);
}

static const char* remoteToken(void* data)
{
    return tic_account_token(data);
}

static void remoteUnauthorized(void* data, const char* token)
{
    tic_account* account = data;

    // Only the current token's 401 signs out: an answer to a request sent
    // before a sign-out or a new pairing must not clear what came after it.
    if(token && strcmp(token, account->token) == 0)
        tic_account_unauthorized(account);
}

static void saveAccount(tic_account* account)
{
    char json[TOKEN_SIZE + NAME_SIZE + 32];
    // Root-relative: a pairing may run with any working directory current.
    tic_fs_makedir(account->fs, "/" TIC_LOCAL);

    s32 size = snprintf(json, sizeof json, "{\"token\":\"%s\",\"user\":\"%s\"}",
                        account->token, account->user);

    tic_fs_saveroot(account->fs, TIC_ACCOUNT_PATH, json, size, true);
}

static void loadAccount(tic_account* account)
{
    s32 size = 0;
    char* json = tic_fs_loadroot(account->fs, TIC_ACCOUNT_PATH, &size);

    if(json)
    {
        if(json_parse(json, size))
        {
            json_string("token", 0, account->token, sizeof account->token);
            json_string("user", 0, account->user, sizeof account->user);
        }

        free(json);
    }
}

tic_account* tic_account_create(tic_fs* fs, tic_net* net)
{
    tic_account* account = calloc(1, sizeof(tic_account));
    account->fs = fs;
    account->net = net;

    loadAccount(account);

    tic_fs_set_remote(fs, &(tic_fs_remote){
                               .signed_in = remoteSignedIn,
                               .token = remoteToken,
                               .unauthorized = remoteUnauthorized,
                               .data = account,
                           });

    return account;
}

void tic_account_close(tic_account* account)
{
    free(account);
}

static void resetPairing(tic_account* account)
{
    account->pairing = PairingIdle;
    account->inFlight = false;
    account->waited = 0;
    account->callback = NULL;
    account->data = NULL;
    account->generation++;
}

// The pairing ends with one answer: the state is dropped before the callback
// runs, so the callback may start another login.
static void finishPairing(tic_account* account, account_pairing_state state, const char* text)
{
    account_pairing_callback callback = account->callback;
    void* data = account->data;

    resetPairing(account);

    if(callback)
        callback(state, text, data);
}

static void onDeviceStart(const net_request_data* response)
{
    AccountOp* op = response->calldata;
    tic_account* account = op->account;

    if(account->generation != op->generation || account->pairing != PairingWaiting)
    {
        free(op);
        return;
    }

    account->inFlight = false;

    if(response->type == net_request_done && response->done.status == 201 && response->done.data)
    {
        const char* body = (const char*)response->done.data;

        if(json_parse(body, response->done.size) &&
           json_has("device_code", 0) && json_string("device_code", 0, account->deviceCode, sizeof account->deviceCode) &&
           json_has("user_code", 0) && json_string("user_code", 0, account->userCode, sizeof account->userCode) &&
           json_has("verification_uri", 0) && json_string("verification_uri", 0, account->uri, sizeof account->uri))
        {
            s32 interval = json_int("interval", 0);
            account->intervalMs = (interval > 0 ? (u32)interval : 3) * 1000;
            account->waited = 0;

            char text[URI_SIZE + CODE_SIZE + 2];
            snprintf(text, sizeof text, "%s\n%s", account->uri, account->userCode);
            account->callback(account_pairing_code, text, account->data);

            free(op);
            return;
        }
    }

    const char* message = NULL;
    char error[128] = {0};

    if(response->type == net_request_done)
    {
        const char* body = (const char*)response->done.data;

        if(body && json_parse(body, response->done.size) &&
           json_has("error", 0) && json_string("error", 0, error, sizeof error))
        {
            message = error;
        }
    }

    finishPairing(account, account_pairing_error, message);
    free(op);
}

static void onPoll(const net_request_data* response)
{
    AccountOp* op = response->calldata;
    tic_account* account = op->account;

    if(account->generation != op->generation || account->pairing != PairingWaiting)
    {
        free(op);
        return;
    }

    account->inFlight = false;

    if(response->type == net_request_done)
    {
        if(response->done.status == 404)
        {
            finishPairing(account, account_pairing_expired, NULL);
            free(op);
            return;
        }

        if(response->done.status == 200 && response->done.data)
        {
            const char* body = (const char*)response->done.data;
            char status[32] = {0};

            if(json_parse(body, response->done.size) && json_has("status", 0) && json_string("status", 0, status, sizeof status))
            {
                if(strcmp(status, "pending") == 0)
                {
                    free(op);
                    return;
                }

                if(strcmp(status, "approved") == 0 &&
                   json_has("token", 0) && json_string("token", 0, account->token, sizeof account->token) &&
                   json_has("user", 0) && json_string("user", 0, account->user, sizeof account->user))
                {
                    saveAccount(account);
                    finishPairing(account, account_pairing_approved, account->user);
                    free(op);
                    return;
                }

                if(strcmp(status, "denied") == 0)
                {
                    finishPairing(account, account_pairing_denied, NULL);
                    free(op);
                    return;
                }
            }
        }

        char error[128] = {0};
        const char* body = (const char*)response->done.data;

        if(body && json_parse(body, response->done.size) &&
           json_has("error", 0) && json_string("error", 0, error, sizeof error))
        {
            finishPairing(account, account_pairing_error, error);
        }
        else
        {
            finishPairing(account, account_pairing_error, NULL);
        }
    }
    else
    {
        finishPairing(account, account_pairing_error, NULL);
    }

    free(op);
}

bool tic_account_login(tic_account* account, account_pairing_callback callback, void* data)
{
    if(tic_account_signedin(account))
    {
        callback(account_pairing_approved, account->user, data);
        return true;
    }

    if(account->pairing != PairingIdle)
        return false;

    account->pairing = PairingWaiting;
    account->inFlight = true;
    account->waited = 0;
    account->callback = callback;
    account->data = data;
    account->generation++;

    char label[128];
    deviceLabel(label, sizeof label);

    char encoded[384];
    urlEncode(encoded, sizeof encoded, label);

    // The request's body and header live until the callback, so they travel
    // with the operation rather than on this stack.
    StartOp* start = calloc(1, sizeof(StartOp));
    start->op.account = account;
    start->op.generation = account->generation;
    start->header = (net_header){"Content-Type", "application/x-www-form-urlencoded"};
    snprintf(start->body, sizeof start->body, "name=%s", encoded);

    tic_net_request(account->net, net_method_post, "/api/v1/client/device",
                    &start->header, 1, start->body, (s32)strlen(start->body), onDeviceStart, &start->op);

    return true;
}

void tic_account_update(tic_account* account, u32 deltaMs)
{
    if(account->pairing != PairingWaiting || account->inFlight)
        return;

    account->waited += deltaMs;

    if(account->waited < account->intervalMs)
        return;

    account->waited = 0;
    account->inFlight = true;

    char url[TOKEN_SIZE + 32];
    snprintf(url, sizeof url, "/api/v1/client/device/%s", account->deviceCode);

    AccountOp* op = calloc(1, sizeof(AccountOp));
    op->account = account;
    op->generation = account->generation;

    tic_net_request(account->net, net_method_get, url, NULL, 0, NULL, 0, onPoll, op);
}

void tic_account_cancel_login(tic_account* account)
{
    if(account->pairing != PairingIdle)
        resetPairing(account);
}

bool tic_account_login_active(tic_account* account)
{
    return account && account->pairing != PairingIdle;
}

// The sign-out's request context carries the header's value, which has to
// outlive tic_net_request.
typedef struct
{
    net_header header;
    char value[TOKEN_SIZE + 16];
} LogoutOp;

static void onLogout(const net_request_data* response)
{
    (void)response;
    free(response->calldata);
}

static void signOut(tic_account* account, bool revoke)
{
    char token[TOKEN_SIZE];
    snprintf(token, sizeof token, "%s", account->token);

    account->token[0] = '\0';
    account->user[0] = '\0';
    resetPairing(account);
    saveAccount(account);

    if(!revoke || token[0] == '\0')
        return;

    LogoutOp* op = calloc(1, sizeof(LogoutOp));
    snprintf(op->value, sizeof op->value, "Bearer %s", token);
    op->header = (net_header){"Authorization", op->value};

    tic_net_request(account->net, net_method_delete, "/api/v1/client/token",
                    &op->header, 1, NULL, 0, onLogout, op);
}

void tic_account_logout(tic_account* account)
{
    signOut(account, true);
}

void tic_account_unauthorized(tic_account* account)
{
    signOut(account, false);
}
